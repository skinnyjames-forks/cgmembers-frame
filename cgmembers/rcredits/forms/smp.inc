<?php
namespace CG\Web;
use CG as r;
use CG\Backend as be;
use CG\Util as u;
use CG\Db as db;
use CG\Txs as x;
use CG\Admin as a;
use CG\Risk as k;
use CG\Web as w;

/**
 * API for interaction with the SMT system (we hope it's general enough for interaction
 * with other systems).
 *
 * @param string $arg: NULL
 * @param $_GET['request']: a JSON object
 *
 * All requests have the same basic format:
 * { "op": whatever the operation is
 *   "userId": the userId of the requester (a company)
 *   "password": password
 *   "requests": a JSON array of objects; each request has a format based on "op"
 * }
 *
 * All responses have the same basic format:
 * { "op": the incoming request op with "-response" tacked on to the end,
 *   "userId": the userId of the requester (for verification),
 *   "status": 'OK' or 'BAD',
 *   "error": only present if status=="BAD", an explanatory message
 *   "responses": only present if status=="OK", an array of responses, one for each element
 *                in the original requests array; each response has a format based on "op"
 */
function smp($arg) {
  global $mya;
  global $channel; $channel = TX_API;

  $data = u\dejsonize(nni($arg ?: $_GET, 'request'));
  extract(just('op userId password requests', $data, NULL));
  $mya = r\acct($userId);
  if (!$mya or !$mya->co or $password != $mya->emailCode) {
    return errorResponse($op, $userId, "Company id $userId not found or wrong password");
  }
  if (is_null($requests) or ! is_array($requests) or $requests == []) {
    return errorResponse($op, $userId, "Improperly formed request");
  }

  switch($op) {
  case 'validate-user-ids':  return validateUserIds($userId, $requests);
  case 'sign-up-users':      return signUpUsers($userId, $requests);
  case 'generate-invoices':  return generateInvoices($userId, $requests);
  case 'make-payments':      return makePayments($userId, $requests);
  case 'get-invoice-status': return getInvoiceStatus($userId, $requests);
  default:
    return errorResponse($op, $userId, 'Invalid API operation');
  }
}

function errorResponse($op, $userId, $error, $status = 'BAD') {
  $op .= '-response';
  $response = compact(ray('op userId error status'));
  header('Access-Control-Allow-Origin: *'); // allow access from web browsers
  return exitJson($response);
}

function okResponse($op, $userId, $responses, $status = 'OK') {
  $op .= '-response';
  $response = compact(ray('op userId status responses'));
  header('Access-Control-Allow-Origin: *'); // allow access from web browsers
  return exitJson($response);
}

function validateUserIds($userId, $requests) {
  $responses = [];
  foreach ($requests as $request) {
    $responses[] = validateUserId($userId, $request);
  }
  return okResponse('validate-user-ids', $userId, $responses);
}

function validateUserId($userId, $request) {
  global $mya;
  extract(just('cgId fullName email phone address city state zipCode', $request, null));
  $userA = r\acct($cgId);
  if (! $userA) { return [ 'status' => 'BAD', 'error' => 'Common Good Account not found', 'cgId' => $cgId]; }
  if (! u\similar($userA->fullName, $fullName, 5, .5) or !($userA->phone == u\fmtPhone(nn($phone), '+n') or $userA->email == $email)) {
    return [ 'status' => 'BAD', 'error' => t('That does not appear to be your correct %PROJECT member ID.'), 'cgId' => $cgId];
  }
  return [ 'status' => 'OK', 'cgId' => $cgId ];
}

function signUpUsers($userId, $requests) {
  $responses = [];
  foreach ($requests as $request) {
    $responses[] = signUpUser($userId, $request);
  }
  return okResponse('sign-up-users', $userId, $responses);
}

function signUpUser($userId, $request) {
  global $mya;
  extract(just('fullName email phone address city state zip_code userPassword nonce', $request, null));
  $zip = $zip_code;
  $newa = new r\Acct(compact(ray('fullName email phone address city state zip')));
  $newa->update(['pass' => $userPassword]);
  return ['nonce' => $nonce, 'status' => 'OK', 'cgId' => $newa->mainQid];
}

function generateInvoices($userId, $requests) {
  $responses = [];
  foreach($requests as $request) $responses[] = generateInvoice($request);
  return okResponse('generate-invoices', $userId, $responses);
}

function generateInvoice($request) {
  global $mya;

  extract(just('amount payerId billingDate dueDate nonce purpose status', $request, null));
  $payerA = r\acct($payerId);
  if (! $payerA) { return ray('nonce status payerId amount error', $nonce, 'BAD', $payerId, $amount, 'Payer not found'); }
  $existing = db\get('nvid, status', 'tx_requests', ray('amount payer payee purpose created', $amount, $payerA->uid, $mya->uid, $purpose, $billingDate));
  if ($existing) {
    extract(just('nvid status', $existing));
    $status = statusString($status) . '-DUPLICATE';
    if ($status == 'BAD-DUPLICATE') {
      return ray('nonce status payerId amount error', $nonce, 'BAD', $payerId, $amount, 'CG system internal error');
    }
    return ray('nonce status payerId amount cgInvoiceId', $nonce, $status, $payerId, $amount, $nvid);
  }
  // Not a duplicate
  $z = be\invoice($mya, $payerA, $amount, $purpose, FOR_GOODS, ['created' => $billingDate]);
  if ($z->ok) {
    $status = statusString($z->subs['status']);
    if ($status == 'BAD') {
      return ray('nonce status payerId amount error', $nonce, 'BAD', $payerId, $amount, 'CG system internal error');
    }
    $cgInvoiceId = $z->subs['nvid'];
    return compact(ray('nonce status payerId amount cgInvoiceId'));
  } else {
    $status = 'BAD';
    $error = [ $z->index ];
    return compact(ray('nonce status error payerId amount'));
  }
}

function makePayments($userId, $requests) {
  foreach($requests as $request) $responses[] = makePayment($request);
  return okResponse('make-payments', $userId, nn($responses));
}

function makePayment($request) {
  global $mya;
  extract(just('amount payeeId billingDate purpose nonce', $request, null));
  $payeeA = r\acct($payeeId);
  $status = 'BAD';
  $error = 'payee account not found';
  if (! $payeeA) { return compact(ray('nonce status payeeId amount error')); }
  $existing = db\get('xid', 'txs_prime', 'amt=:amt and uid1=:uid1 and uid2=:uid2 and created=:created and for1=:for1',
                     ray('amt uid1 uid2 created for1', $amount, $mya->uid, $payeeA->uid, $billingDate, $purpose));
  if ($existing) {
    $status = 'DUPLICATE';
    return compact(ray('nonce status payeeId amount'));
  }
  $z = be\transfer('payment', $mya, $payeeA, $amount, $purpose, FOR_GOODS, ['created'=>$billingDate]);
  if ($z->ok) return ray('nonce status payeeId amount', $nonce, 'OK', $payeeId, $amount);

  switch ($z->index) {
  case 'short from vague':
  case 'short from':
  case 'short to':
    $status = 'NSF';
    break;
  default:
    $status = 'BAD';
  }
  
  $error = $z->msg;
  return compact(ray('nonce status payeeId amount error'));
}

function getInvoiceStatus($userId, $requests) {
  $responses = [];
  foreach($requests as $request) {
    $responses[] = getOneInvoiceStatus($request);
  }
  return okResponse('get-invoice-status', $userId, $responses);
}

function getOneInvoiceStatus($request) {
  global $mya;
  extract(just('cgInvoiceId amount payerId', $request));
  $payerA = r\acct($payerId);
  if (! $payerA) { return ray('cgInvoiceId amount payerId status error', $cgInvoiceId, $amount, $payerId, 'BAD', 'payer account missing'); }
  $result = db\get('status, payee', 'tx_requests', ['nvid'=>$cgInvoiceId, 'amount'=>$amount, 'payer'=>$payerA->uid, 'payee'=>$mya->uid]);
  if ($result == FALSE) {
    return ray('cgInvoiceId amount payerId status error', $cgInvoiceId, $amount, $payerId, 'BAD', 'no invoice found');
  }
  return ray('cgInvoiceId amount payerId status', $cgInvoiceId, $amount, $payerId, statusString($result['status']));
}

function statusString($status) {
  if ($status >= TX_PAID) return 'PAID';
  switch ($status) {
  case TX_DELAYED: return('DELAYED');
  case TX_DENIED: return('DENIED');
  case TX_PENDING: return('PENDING');
  case TX_APPROVED: return('APPROVED');
  default: return('BAD');
  }
}
