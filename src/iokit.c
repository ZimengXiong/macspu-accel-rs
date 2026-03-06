#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RING_CAP 8000
#define RING_ENTRY 12
#define RING_HEADER 16
#define RING_SIZE (RING_HEADER + RING_CAP * RING_ENTRY)

#define IMU_REPORT_LEN 22
#define IMU_DATA_OFFSET 6
#define IMU_DECIMATION 8
#define REPORT_BUF_SIZE 4096
#define REPORT_INTERVAL_US 1000

#define PAGE_VENDOR 0xFF00
#define USAGE_ACCEL 3
#define USAGE_GYRO 9

#define kCFNumberSInt32Type 3
#define kCFNumberSInt64Type 4

#define MAX_DEVICES 8

static uint8_t g_accel_ring[RING_SIZE];
static uint8_t g_gyro_ring[RING_SIZE];
static int g_accel_decimation_count = 0;
static int g_device_count = 0;
static int g_stop = 0;
static uint8_t g_report_bufs[MAX_DEVICES][REPORT_BUF_SIZE];
static IOHIDDeviceRef g_devices[MAX_DEVICES];

typedef void (*TSReportCallback)(
  void *context,
  IOReturn result,
  void *sender,
  IOHIDReportType type,
  uint32_t reportID,
  uint8_t *report,
  CFIndex reportLength
);

static void ring_write_sample(uint8_t *ring, int32_t x, int32_t y, int32_t z) {
  uint32_t idx;
  memcpy(&idx, &ring[0], 4);

  size_t off = RING_HEADER + (size_t)idx * RING_ENTRY;
  memcpy(&ring[off], &x, 4);
  memcpy(&ring[off + 4], &y, 4);
  memcpy(&ring[off + 8], &z, 4);

  uint32_t next_idx = (idx + 1) % RING_CAP;
  memcpy(&ring[0], &next_idx, 4);

  uint64_t total;
  memcpy(&total, &ring[4], 8);
  total++;
  memcpy(&ring[4], &total, 8);
}

static void imu_callback(void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {
  (void)result;
  (void)sender;
  (void)type;
  (void)reportID;
  if (g_stop) {
    return;
  }
  if (reportLength != IMU_REPORT_LEN) {
    return;
  }

  int32_t x, y, z;
  memcpy(&x, &report[IMU_DATA_OFFSET], 4);
  memcpy(&y, &report[IMU_DATA_OFFSET + 4], 4);
  memcpy(&z, &report[IMU_DATA_OFFSET + 8], 4);

  intptr_t usage = (intptr_t)context;
  if (usage == USAGE_ACCEL) {
    g_accel_decimation_count++;
    if (g_accel_decimation_count < IMU_DECIMATION) {
      return;
    }
    g_accel_decimation_count = 0;
    ring_write_sample(g_accel_ring, x, y, z);
  } else if (usage == USAGE_GYRO) {
    ring_write_sample(g_gyro_ring, x, y, z);
  }
}

static void set_int_property(io_service_t service, CFStringRef key, int32_t value) {
  CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
  if (num) {
    IORegistryEntrySetCFProperty(service, key, num);
    CFRelease(num);
  }
}

static int64_t get_int_property(io_service_t service, CFStringRef key) {
  int64_t val = 0;
  CFTypeRef prop = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0);
  if (prop) {
    CFNumberGetValue(prop, kCFNumberSInt64Type, &val);
    CFRelease(prop);
  }
  return val;
}

static int wake_spu_drivers(void) {
  CFMutableDictionaryRef matching = IOServiceMatching("AppleSPUHIDDriver");
  io_iterator_t iter;
  kern_return_t kr = IOServiceGetMatchingServices(0, matching, &iter);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "tiltball spu: AppleSPUHIDDriver matching failed (kr=%d)\n", kr);
    return -1;
  }

  int found = 0;
  io_service_t svc;
  while ((svc = IOIteratorNext(iter)) != 0) {
    found = 1;
    set_int_property(svc, CFSTR("SensorPropertyReportingState"), 1);
    set_int_property(svc, CFSTR("SensorPropertyPowerState"), 1);
    set_int_property(svc, CFSTR("ReportInterval"), REPORT_INTERVAL_US);
    IOObjectRelease(svc);
  }
  IOObjectRelease(iter);
  if (!found) {
    fprintf(stderr, "tiltball spu: no AppleSPUHIDDriver services found\n");
  }
  return found ? 0 : -1;
}

static int register_hid_devices(void) {
  CFMutableDictionaryRef matching = IOServiceMatching("AppleSPUHIDDevice");
  io_iterator_t iter;
  kern_return_t kr = IOServiceGetMatchingServices(0, matching, &iter);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "tiltball spu: AppleSPUHIDDevice matching failed (kr=%d)\n", kr);
    return -1;
  }

  int callbacks = 0;
  int candidates = 0;
  io_service_t svc;
  while ((svc = IOIteratorNext(iter)) != 0) {
    int64_t usage_page = get_int_property(svc, CFSTR("PrimaryUsagePage"));
    int64_t usage = get_int_property(svc, CFSTR("PrimaryUsage"));

    if (usage_page == PAGE_VENDOR && (usage == USAGE_ACCEL || usage == USAGE_GYRO) && g_device_count < MAX_DEVICES) {
      candidates++;
      IOHIDDeviceRef hid = IOHIDDeviceCreate(kCFAllocatorDefault, svc);
      if (hid) {
        if (IOHIDDeviceOpen(hid, kIOHIDOptionsTypeNone) == kIOReturnSuccess) {
          int idx = g_device_count;
          g_devices[idx] = hid;
          g_device_count++;
          IOHIDDeviceRegisterInputReportCallback(hid, g_report_bufs[idx], REPORT_BUF_SIZE, imu_callback, (void *)(intptr_t)usage);
          IOHIDDeviceScheduleWithRunLoop(hid, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
          callbacks++;
        } else {
          CFRelease(hid);
        }
      }
    }

    IOObjectRelease(svc);
  }

  IOObjectRelease(iter);
  fprintf(stderr, "tiltball spu: accel candidates=%d callbacks=%d\n", candidates, callbacks);
  return callbacks > 0 ? 0 : -1;
}

static void cleanup_devices(void) {
  for (int i = 0; i < g_device_count; i++) {
    if (g_devices[i]) {
      IOHIDDeviceUnscheduleFromRunLoop(g_devices[i], CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
      IOHIDDeviceClose(g_devices[i], kIOHIDOptionsTypeNone);
      CFRelease(g_devices[i]);
      g_devices[i] = NULL;
    }
  }
  g_device_count = 0;
}

int iokit_sensor_init(void) {
  memset(g_accel_ring, 0, RING_SIZE);
  memset(g_gyro_ring, 0, RING_SIZE);
  g_accel_decimation_count = 0;
  g_device_count = 0;
  g_stop = 0;
  memset(g_devices, 0, sizeof(g_devices));
  return 0;
}

void iokit_sensor_stop(void) { g_stop = 1; }

void iokit_sensor_run(void) {
  if (wake_spu_drivers() != 0) {
    return;
  }
  if (register_hid_devices() != 0) {
    cleanup_devices();
    return;
  }

  while (!g_stop) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
  }

  cleanup_devices();
}

const uint8_t *iokit_ring_ptr(void) { return g_accel_ring; }

const uint8_t *iokit_gyro_ring_ptr(void) { return g_gyro_ring; }
