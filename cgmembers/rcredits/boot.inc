<?php
/**
 * @file
 * Set general environment and operational settings
 * Also used by /do.php, for no-sign-in database changes
 */

strip($_POST); strip($_GET); // No HTML input EVER (unless encoded in a way we expect)
 
global $base_url, $base_path, $base_root, $styleNonce;
global $databases, $drupal_hash_salt, $cryHows;
global $cookie_domain, $is_https, $base_secure_url, $base_insecure_url;
global $CGdomain;

require __DIR__ . '/defs.inc';
require_once DRUPAL_ROOT . '/includes/errors.inc';

set_exception_handler('handleException');
set_error_handler('handleError');

timer_start('page');

$uri = 'I' . nni($_SERVER, 'REQUEST_URI'); // handle scanned rCard URIs
$_SERVER['REQUEST_URI'] = $_GET['q'] = preg_match('~^I/[A-Z]+[\.-]~', $uri) ? $uri : \request_path();
define('URI', $_SERVER['REQUEST_URI']);
define('PAGE', basename(URI));
    
if (NOT_PRODUCTION) {
  ini_set('display_startup_errors', TRUE);
}
ini_set('display_errors', 0);
ini_set('magic_quotes_runtime', '0'); // Don't escape quotes when reading files from the database, disk, etc.
ini_set('session.use_cookies', '1'); // Use session cookies, not transparent sessions with the session id in the query string.
ini_set('session.use_only_cookies', '1');
ini_set('session.use_trans_sid', '0');
ini_set('session.cache_limiter', ''); // Don't send HTTP headers using PHP's session handler.
ini_set('session.cookie_httponly', '1'); // Use httponly session cookies.

ini_set('error_reporting', E_ALL);
ini_set('max_execution_time', isDEV ? 0 : (4 * 60)); // don't ever timeout processing when developing
ini_set('session.gc_probability', 1); // garbage collection this fraction of the time on startup (numerator)
ini_set('session.gc_divisor', 0); // (denominator) - disable random garbage collection (do it in cron instead)
define('SESSION_LIFE', (nni($_COOKIE, 'pw2') or nni($_POST, 'pw2')) ? (12 * HOUR_SECS) : (24 * MIN_SECS)); // standard 24 mins (more for admin)
//define('SESSION_LIFE', (nni($_COOKIE, 'pw2') or nni($_POST, 'pw2')) ? (5) : (2 * MIN_SECS)); // for testing
ini_set('session.gc_maxlifetime', SESSION_LIFE); // session timeout (and garbage collection happens)
ini_set('session.cookie_lifetime', 0); // SESSION_LIFE); // (drupal doesn't refresh the cookie)

setlocale(LC_ALL, 'C'); // Set sane locale settings, to ensure consistent string, dates, times and numbers handling.

$drupal_hash_salt = $db_salt;

$is_https = isset($_SERVER['HTTPS']) && strtolower($_SERVER['HTTPS']) == 'on';

$base_secure_url = str_replace('http://', 'https://', $base_url);
$base_insecure_url = str_replace('https://', 'http://', $base_url);

// Strip leading periods, www., and port numbers from cookie domain.
$cookie_domain = ltrim($host, '.');
if (strpos($cookie_domain, 'www.') === 0) $cookie_domain = substr($cookie_domain, 4);
$cookie_domain = explode(':', $cookie_domain);
$cookie_domain = '.' . $cookie_domain[0];

// use $base_url, without the protocol, as session name, to use the same session identifiers across HTTP and HTTPS.
//list( , $session_name) = explode('://', $base_url, 2);
$session_name = $cookie_domain;
// To prevent session cookies from being hijacked, a user can configure the
// SSL version of their website to only transfer session cookies via SSL by
// using PHP's session.cookie_secure setting. The browser will then use two
// separate session cookies for the HTTPS and HTTP versions of the site. So we
// must use different session identifiers for HTTPS and HTTP to prevent a
// cookie collision.
if ($is_https) ini_set('session.cookie_secure', TRUE);
$prefix = $is_https ? 'SSESS' : 'SESS';

session_name($prefix . substr(hash('sha256', $session_name), 0, 32));

// Per RFC 2109, cookie domains must contain at least one dot other than the
// first. For hosts such as 'localhost' or IP Addresses we don't set a cookie domain.
if (count(explode('.', $cookie_domain)) > 2 && !is_numeric(str_replace('.', '', $cookie_domain))) {
  ini_set('session.cookie_domain', $cookie_domain);
}

$agt = nni($_SERVER, 'HTTP_USER_AGENT');
define('isSAFARI', stripos($agt, 'Chrome') === FALSE and stripos($agt, 'Safari') !== FALSE);

/**
 * Remove any (potentially hazardous) HTML from input.
 * Specifically, remove <, followed by ! or any letter, through the closing > or end of string.
 */
function strip(&$v) {
  if (is_array($v)) {
    foreach ($v as $k => $v2) strip($v[$k]);
  } else $v = preg_replace('~<[!/a-z][^>]*(>|$)~i', '', $v); // not strip_tags, which zaps <3, etc.
}

define('FRAME_SOURCES', "https://cg4.us/chart.php $base_url/rcredits/misc/chart.php $base_url/sadmin/export-list/ $base_url/show-doc/"); // base_url not self, because we use this to sanitize page too

/** Set Content-Security-Policy
 * We might want to use require-sri-for eventually for even our own scripts and style sheets
 * 'sha256-47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=' for inline style in spin.min.js
 * 'sha256-2EA12+9d+s6rrc0rkdIjfmjbh6p2o0ZSXs4wbZuk/tA=' for inline style in jquery-3.1.1.min.js
 * 'sha256-nX7L7GcvxmPKgeYAFQSzFcLplU7F/i/72ytdtLdJYoI=' for jquery also
 * 'sha256-+t47tgldASBy5zANml11C5vRg3a/7oPrKstKXlzZfkI=' for jquery also
 * 'sha256-+7g9GIVwIQyRW5AWmV3tOknRu/VejUoNtGLu4+COYXU=' for inline style in chartist.min.js
 * nonces are for spin.min.js on iPhone (but it doesn't work)
 */
function setCSP() {
  global $styleNonce; $styleNonce = base64_encode(substr(hash('sha256', PROJECT . now(), TRUE), 0, 32));
  $sid = session_id();
  $uri = $_SERVER['REQUEST_URI'];

  $styleSource = ($uri == 'co/cgbutton' or ($uri == 'community/invite' and nni($_POST, 'example')))
  ? "'unsafe-inline'"  // allow inline styles when creating button
  : "'nonce-$styleNonce' 'sha256-47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=' 'sha256-+t47tgldASBy5zANml11C5vRg3a/7oPrKstKXlzZfkI=' 'sha256-nX7L7GcvxmPKgeYAFQSzFcLplU7F/i/72ytdtLdJYoI=' 'sha256-2EA12+9d+s6rrc0rkdIjfmjbh6p2o0ZSXs4wbZuk/tA='";
  
  header('Content-Security-Policy: '
    . "base-uri 'self';"
    . "block-all-mixed-content;"
    . "default-src 'self';"
    . "connect-src 'self';"
    . "font-src 'self';"
    . tr("frame-src %FRAME_SOURCES;") // we use iframes for admin member download and for graphs
    . "manifest-src 'self';"
    . "media-src 'self';"
    . "object-src 'none';"
    . "script-src 'self' 'nonce-$styleNonce' 'report-sample';" // 'report-sample' allows script fragment to be included in CSP reports
    . "img-src 'self' https://cg4.us data:;" // data: is for photo upload and QR code
    . "style-src 'self' $styleSource;"
    . "worker-src 'self';"
    . "form-action 'self';"
    . "frame-ancestors 'self';"
//    . "report-uri /csp-report.php?sid=$sid;"
  );

  header('X-Frame-Options: DENY');
  // Use <iframe sandbox="allow-scripts" ...> for cg4.us frame showing charts

}
