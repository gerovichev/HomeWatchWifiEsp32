<?php
// Configuration options
$debug_mode = false; // Set to true for verbose logging
$firmware_dir = "./bin/"; // Directory containing firmware files

// Custom logging function - uses nginx error.log via PHP error_log()
function debug_log($message) {
    global $debug_mode;
    $timestamp = date('Y-m-d H:i:s');
    $log_message = "[OTA] [$timestamp] $message";
    
    // Always log to nginx error.log via PHP error_log()
    // This will appear in nginx error.log (usually /var/log/nginx/error.log)
    error_log($log_message);
    
    // Also log to PHP error log if debug mode is enabled (for additional verbosity)
    if ($debug_mode) {
        error_log($log_message, 0); // Type 0 = system logger (syslog/nginx error.log)
    }
}

// Initialize error handling
ini_set('display_errors', 0);
ini_set('log_errors', 1);

/**
 * Send a specified file with appropriate headers.
 *
 * @param string $path
 * @return void
 */
function sendFile($path) {
    if (!file_exists($path)) {
        debug_log("File not found: $path");
        http_response_code(404);
        exit("File not found");
    }
    
    debug_log("Sending file: $path (Size: " . filesize($path) . " bytes)");
    header($_SERVER["SERVER_PROTOCOL"] . ' 200 OK', true, 200);
    header('Content-Type: application/octet-stream', true);
    header('Content-Disposition: attachment; filename=' . basename($path));
    header('Content-Length: ' . filesize($path), true);
    header('x-MD5: ' . md5_file($path), true);
    readfile($path);
    exit();
}

// Detect device type: ESP8266 uses headers (check first as more specific), ESP32 uses GET parameters
$is_esp8266 = isset($_SERVER['HTTP_X_ESP8266_STA_MAC']);
$is_esp32 = !$is_esp8266 && (isset($_GET['MAC']) || isset($_GET['ver']) || isset($_GET['hst']));

// ========== ESP8266 HANDLING (check first as more specific) ==========
if ($is_esp8266) {
    // ESP8266 uses HTTP headers
    header('Content-type: text/plain; charset=utf8', true);
    
    // Check for required headers (make some optional for compatibility)
    $required_headers = [
        'HTTP_X_ESP8266_STA_MAC',  // Required
    ];
    
    $missing_required = [];
    foreach ($required_headers as $header) {
        if (!isset($_SERVER[$header])) {
            $missing_required[] = $header;
        }
    }
    
    if (!empty($missing_required)) {
        debug_log("Missing required ESP8266 headers: " . implode(', ', $missing_required));
        http_response_code(403);
        exit("Only for ESP8266 updater! (missing headers)\n");
    }
    
    // Get device info (don't log yet - only log if update needed or error)
    $mac = $_SERVER['HTTP_X_ESP8266_STA_MAC'];
    $free_space = isset($_SERVER['HTTP_X_ESP8266_FREE_SPACE']) ? $_SERVER['HTTP_X_ESP8266_FREE_SPACE'] : 'unknown';
    $current_md5 = isset($_SERVER['HTTP_X_ESP8266_SKETCH_MD5']) ? $_SERVER['HTTP_X_ESP8266_SKETCH_MD5'] : '';
    
    // Define MAC address to file mapping for ESP8266
    $db_esp8266 = [
        "EC:FA:BC:0C:D5:25" => "HomeWatchWifi.ino", //mamad
        "E8:DB:84:96:C0:17" => "HomeWatchWifi.ino", //parent
        "E8:DB:84:93:FA:6C" => "HomeWatchWifi.ino",
        "84:F3:EB:0C:A4:2E" => "HomeWatchWifi.ino",
        "E8:DB:84:94:98:98" => "HomeWatchWifi.ino",
        "68:C6:3A:D6:BC:7F" => "HomeWatchWifi.ino",
        "5C:CF:7F:33:91:28" => "HomeWatchWifi.ino",
        "DC:4F:22:4C:8B:D7" => "HomeWatchWifi.ino",
        "84:F3:EB:9F:6D:15" => "HomeWatchWifi.ino",
    ];
    
    // Check if MAC address is configured
    if (!isset($db_esp8266[$mac])) {
        debug_log("ESP8266 MAC not configured: $mac");
        http_response_code(403);
        exit("Device not authorized for updates");
    }
    
    // Construct path to binary file
    $firmware_name = $db_esp8266[$mac];
    $localBinary = $firmware_dir . $firmware_name . ".bin";
    
    // Check if the local binary file exists
    if (!file_exists($localBinary)) {
        debug_log("ESP8266 Binary file not found: $localBinary");
        http_response_code(404);
        exit("Firmware not available");
    }
    
    // Compare MD5 hash and send file if they don't match
    // If MD5 header is not present, always send file (safer for updates)
    $localBinaryMD5 = md5_file($localBinary);
    
    if (empty($current_md5)) {
        // MD5 not provided - log and send file
        debug_log("ESP8266 Device MAC: $mac, Free space: $free_space, MD5 not provided - sending firmware");
        sendFile($localBinary);
    } elseif ($current_md5 !== $localBinaryMD5) {
        // MD5 mismatch - log and send file
        debug_log("ESP8266 Firmware update needed. MAC: $mac, Free space: $free_space, Current MD5: $current_md5, New MD5: $localBinaryMD5");
        sendFile($localBinary);
    } else {
        // MD5 matches - no update needed, no logging for 304
        http_response_code(304); // Not Modified
        exit("No update needed");
    }
    exit; // Should not reach here
}

// ========== ESP32 HANDLING ==========
if ($is_esp32) {
    // ESP32 sends parameters via GET: MAC, hst (hostname), ip, ver (version)
    $mac = isset($_GET['MAC']) ? trim($_GET['MAC']) : '';
    $hostname = isset($_GET['hst']) ? trim($_GET['hst']) : '';
    $ip = isset($_GET['ip']) ? $_GET['ip'] : '';
    $device_version = isset($_GET['ver']) ? trim($_GET['ver']) : '';
    
    // Don't log request yet - only log if update needed or error
    
    // Define MAC address to file mapping for ESP32
    $db_esp32 = [
        "B0:CB:D8:04:7A:F0" => 'HomeWatchWifiEsp32.ino.bin',
        "84:F7:03:0B:CC:5C" => 'HomeWatchWifiEsp32c3.ino.bin',  // ESP_Parent_WEB_Mamad
        // Add more ESP32 devices here
        // "AA:BB:CC:DD:EE:FF" => 'HomeWatchWifiEsp32_Device2.ino.bin',
    ];
    
    // Check if MAC address is configured
    if (!isset($db_esp32[$mac])) {
        debug_log("ESP32 MAC not configured: $mac");
        http_response_code(403);
        exit("Device not authorized for updates");
    }
    
    $firmware_name = $db_esp32[$mac];
    $localBinary = $firmware_dir . $firmware_name;
    
    // Check if file exists
    if (!file_exists($localBinary)) {
        debug_log("ESP32 Binary file not found: $localBinary");
        http_response_code(404);
        exit("Firmware not available");
    }
    
    // Auto-detect version from filename or file modification time
    // Try to extract version from filename (e.g., "firmware_260115.bin" -> "260115")
    $server_version = null;
    if (preg_match('/(\d{6,})/', basename($firmware_name), $matches)) {
        $server_version = $matches[1];
    } else {
        // Fallback: use file modification timestamp as version (YYYYMMDDHHMMSS format)
        $server_version = date('ymd', filemtime($localBinary));
    }
    
    // Compare versions (ESP32 uses version string comparison)
    // Normalize to digits only to avoid loops when server uses filemtime timestamps.
    $device_ver_norm = preg_replace('/\D+/', '', $device_version);
    $server_ver_norm = preg_replace('/\D+/', '', $server_version);

    // If device uses short version (e.g. yymmdd), compare only that length.
    if ($device_ver_norm !== '' && strlen($device_ver_norm) < strlen($server_ver_norm)) {
        $server_ver_norm = substr($server_ver_norm, 0, strlen($device_ver_norm));
    }

    $device_ver_int = (int)$device_ver_norm;
    $server_ver_int = (int)$server_ver_norm;
    
    if ($device_ver_int < $server_ver_int) {
        debug_log("ESP32 Firmware update needed. Device version: $device_version, Server version: $server_version");
        sendFile($localBinary);
    } else {
        // No logging for 304 - device is up to date, no action needed
        http_response_code(304); // Not Modified
        header('Content-Type: text/plain');
        exit("No update needed");
    }
    exit; // Should not reach here, but just in case
}


// ========== UNKNOWN DEVICE ==========
// If we reach here, it's neither ESP32 nor ESP8266
debug_log("Unknown device type - no ESP32 GET params, no ESP8266 headers");
http_response_code(400);
header('Content-Type: text/plain');
exit("Invalid request format\n");
?>
