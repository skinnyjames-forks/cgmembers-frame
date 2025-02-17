<?php
use CG\Util as u;
use CG\QR as qr;
use Ratchet\MessageComponentInterface;
use Ratchet\ConnectionInterface;
use Ratchet\Server\IoServer;
use Ratchet\Http\HttpServer;
use Ratchet\WebSocket\WsServer;
use React\Socket\Server;
use React\Socket\SecureServer;

/**
 * @file
 * Switchboard to route messages from one instance of the CGPay app to another.
 * Most obviously: "I request that you pay me $x for whatever." (and the yes/no response)
 * Run in crontab with: <star>/5 * * * * php /home/new/cgmembers-frame/current/cgmembers/rcredits/rcron/app-socket.php  > /dev/null 2>&1 &
 *
 * Parameters for messaging:
 *   op deviceId actorId otherId action amount description created note
 *   op: conn or tell
 *   deviceId: the device's assigned ID (must be associated in r_boxes with actorId)
 *   actorId: the sender's QR-style account ID
 *   otherId: recipient account ID
 *   action: paid, charged, request, denied
 *   amount: transaction or request amount
 *   description: transaction (or request) description
 *   created: transaction or invoice creation date
 *   note: if action is "denied", this is the reason
 */
define('DRUPAL_ROOT', __DIR__ . '/../..');
require_once __DIR__ . '/../bootstrap.inc';
require_once DRUPAL_ROOT . '/rcredits/boot.inc';
require_once DRUPAL_ROOT . '/../vendor/autoload.php';
require_once R_ROOT . '/cg-util.inc';
require_once R_ROOT . '/cg-qr.inc';

global $channel; $channel = TX_SOCKET; // set this even if called from PHP window by admin (must be after bootstrapping)
ignore_user_abort(TRUE); // Allow execution to continue even if the request gets canceled.
set_time_limit(0);
$GLOBALS['user'] = \drupal_anonymous_user();

class MyWSSServer implements MessageComponentInterface {
  protected $clients;
  protected $map; // maps account IDs to connections
  
  public function __construct() {$this->clients = new \SplObjectStorage;}
  public function onOpen(ConnectionInterface $conn) {$this->clients->attach($conn);}
  public function onClose(ConnectionInterface $conn) {$this->clients->detach($conn);}
  public function onError(ConnectionInterface $conn, \Exception $e) {return er($e->getMessage());}

  /**
   * Handle an incoming message.
   * @param conn $from: what connection the message came in on
   * @param string $msg: JSON-encoded message
   */
  public function onMessage(ConnectionInterface $from, $msg) {
    if (!$ray = json_decode($msg)) return er(t('Bad JSON message: ') . pr($msg), $from);
    flog('app socket got: ' . pr($ray));
    extract(just('op deviceId actorId otherId name action amount purpose note', $ray, NULL)); // op, deviceId, and actorId are always required
    if (!$qid = qr\qid($actorId)) return er(t('"%actorId" is not a recognized actorId.', compact('actorId')), $from);
    $ok = ( ($deviceId == bin2hex(R_WORD)) // called from u\tellApp
    or (!isPRODUCTION and $deviceId == 'dev' . substr($qid, -1)) // testing (for example devA)
    or ($x = u\decryPGP(u\b64decode($deviceId), 'public') and strstr($x, '/', TRUE) == $qid) );
    if (!$ok) return er(t('"%actorId" is not an authorized account.', compact('actorId')), $from); // server sends R_WORD instead of deviceId
    
    switch ($op) {
      case 'connect': 
//        $firstTime = empty($oldFrom = nni($this->map, $actorId));
        $this->map[$actorId] = $from; 
/*        $action = $note = '';
        $message = t('You are now connected to the socket switchboard (account %actorId).', compact('actorId'));
        if ($firstTime) $from->send(json_encode(compact(ray('message action note')))); 
        flog(t('firstTime=') . ($firstTime ? 1 : 0));
        */
        break;
      case 'tell': // currently this comes only from u/tellApp()
        if (!$to = nni($this->map, $otherId)) return;
        $what = t('%amt for %what', 'amt what', u\fmtAmt($amount), $purpose);
        $subs = ray('name action what note', $name, $action, $what, $note);
        $message = $action == 'request' ? t('%name asks you to pay %what. Okay?', $subs)
        : ($action == 'denied' ? t('%name has denied your request to pay %what, because "%note".', $subs)
        : t('%name %action you %what.', $subs)); // paid or charged
        $to->send(json_encode(compact(ray('message action note'))));
        break;
      default: return er(t('Bad op: ') . $op, $from);
    }
  }
}

/**/ echo $startMsg = fmtDt() . ' ' . fmtTime() . t(" Restarting app websocket switchboard...\n"); // in case we run this from the command line

try {
  set_error_handler('alreadyRunning', E_WARNING); // handle warning about "Address already in use"
  $loop = \React\EventLoop\Factory::create();
  $websockets = new Server('0.0.0.0:' . SOCKET_PORT, $loop);
  restore_error_handler();
  
  $secure_websockets = new SecureServer($websockets, $loop, [
      'local_cert' => '/etc/pki/tls/certs/commongood.earth.pem',
      'local_pk' => '/etc/pki/tls/private/commongood.earth.key',
      'verify_peer' => false,
  ]);

  $app = new HttpServer(new WsServer(new MyWSSServer()));
  $server = new IoServer($app, $secure_websockets, $loop);
  /**/ flog($startMsg);
  $server->run();
} catch (\Exception $er) {
/**/ flog("App socket overall er: " . $er->message());
}

function er($msg, $conn) {
/**/ flog("App socket error: $msg\n");
  $conn->close();
}

function alreadyRunning() {exit("Warning: that port (probably the switchboard) is already running.\n");}
