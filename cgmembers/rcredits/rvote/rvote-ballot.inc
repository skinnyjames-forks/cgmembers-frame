<?php
namespace CG\Vote;
use CG as r;
use CG\DB as db;
use CG\Backend as be;
use CG\Util as u;
use CG\Web as w;
use CG\Vote as v;

define('RECOVERY_OPTS', 
  '<b>Use.</b> Increased Transaction Volume because we will use the Common Good system for payments (in and/or out)|' .
  '<b>Economic Circles!</b> because we plan to pay our employees and suppliers through Common Good, whenever possible, so the funds keep circulating and perhaps never go out of the system at all|' .
  '<b>Invitations.</b> Probable Donations from new Common Good members we invite|' .
  '<b>Roundups.</b> Gradual Donations over the years from Common Good members who pay us, rounding their payments up to the nearest dollar and donate the change|' .
  '<b>Percentage.</b> Gradual Donations over the years, as a tiny percentage of payments we, the Applicant, receive through the Common Good system|' .
  '<b>Dividends</b> (depending on the project\'s financial success)|' .
  '<b>Eventual Repayment of Loan</b>|' .
  '<b>Interest</b> on the loan|' .
  '<b>Other</b>'
);

/* Note that vetoing costs the voter a proportionate share of their votes. 
For example, if there are 10 items to choose from, vetoing any one of them reduces the voter's say in the other 9 by 10%
A comment explaining the rationale for the veto is required. Each voter gets only one veto per question, but the veto may be split among more than one option. For example, if the voter vetoes 3 options, then each veto counts as 1/3 veto.
Multiple choice options are presented in ascending order (most negative on the left) because that gives more accurate data -- see this article: https://www.td.org/insights/evidence-based-survey-design-strongly-agree-on-the-left-or-right-side-of-the-likert-scale
*/

// Election can be verified with a list of voters (without ballot number) and a list of ballots (without voter id).
// List of ballots includes the ballot id, voted grade for each option and each question, and proxy ballot id if not direct.

/**
 * Return one ballot question.
 */
function ballot($mya, &$extras, $question) {
  global $quids;
  
  list($quids, $qutypes) = getQuestions('', $mya);
  if (!$qcount = count($quids)) return t('There is no vote in progress for your community.');
  u\setDft($question, $quids[0]);

  // set up question to display
  $qnum = array_search($question, $quids); // this works even if the number of elections/questions changes during voting
  $qndpy = $qnum + 1; // $qnum starts at 0
  $extras['question'] = w\hidFld($question);
  $submit = w\submit(BARE . t('Done With Question #') . $qndpy, 'primary', 'md', w\clas('pull-right'));
  $extras['prev'] = w\submit('&laquo; ' . t('Previous'), 'primary', 'md', w\suffix(w\rendA('submit', $submit)));

  $qu = v\question($question);
  $tableHeader = tableHeader($qu['type']);
  if($qu['linkDiscussion']) $qu['linkDiscussion'] = "<div class=\"discussionlink\"> [<a href=\"$qu[linkDiscussion]\" target=\"_blank\">See discussion</a>]</div>";
  $range = formatQuestionDates($qu['start'], $qu['end'], 0);

  // get current voted values for this question
  $defaults = getCurrentVote($mya, $extras, $question, $qu['type'], $qu['optOrder']);
///    debug(compact('defaults'));
  $optCount = $defaults['optCount'];
  $opts = getOptsX($defaults['ballot'], $qu['type'], $defaults);

  w\js('ballot');
  
  if(strpos(' BR', $qu['type'])) {
    w\css('x/bootstrap-slider.min.css');
    w\js("x/bootstrap-slider.min");
  }

//<form name='ballotform' id='ballotform' action='ballot.php' method='$METHOD' onsubmit="return checkform(this, $optCount, '$qu[type]');">
  $qu['detail'] = u\parseUp($qu['detail']);

  $bodymiddle = <<<EOF
<div class='ballot well q$qu[type]'>

<div class='ballotheader'>
<div class='electiontitle'>CGVoting: $range</div electiontitle>
<div class='questioncount'>(Question #$qndpy of $qcount)</div questioncount>
<div class='clearer'></div>
</div ballotheader>

$qu[linkDiscussion]
<div class='question'><b>QUESTION #$qndpy:</b> $qu[text]</div question>
<div class='questiondetail'>$qu[detail]</div questiondetail>
<div class='optionheader'>$tableHeader</div optionheader>
<div id='options'>
$opts
</div options>
</div ballot>
EOF;

  return $bodymiddle;
}

/**
 * Display the details of a proposal, optionally along with grading questions.
 * @param acct $mya: the current account (possibly from an email link)
 * @param int $proposal: the proposal id
 * @param string $err: (RETURNED) error message (empty if no error)
 * @param bool $grading: <include grading questions>
 * @return assoc of form fields to add to form, to be displayed
 */
function showProposal($mya, $proposal, &$err = FALSE, $grading = FALSE) {
  global $quids;
  $noneErr = t('There are no active proposals to grade in your community.');
  $qcount = count($quids = getProposals($mya));
  if (!$proposal) $proposal = nni($quids, 0);
  $qnum = array_search($proposal . '', $quids, TRUE); // this works even if the number of elections/proposal changes during grading
  
  if (!$row = db\get('*', 'r_proposals', 'id=:proposal', compact('proposal'))) return !$err = t('There is no such proposal.');
  
  extract(just('project categories overview purpose systemic where when until how amount recovery budgetTotal budget contingency qualifications evaluation name contact phone email sponsor', $row));

  if ($grading) {
    $optType = 'M'; // multiple choice
    $tableHeader = tableHeader($optType);
    if (!$qcount) return !$err = $noneErr;
    $qndpy = $qnum + 1; // $qnum starts at 0

  // get current voted values for this question
    $defaults = getCurrentVote($mya, $extra, -$proposal, $optType, '');
    $optCount = $defaults['optCount'];
    $opts = getOptsX($defaults['ballot'], 'M', $defaults);
    $detail = t('Scroll down to see the proposal. Click a rating twice for X<sup>+</sup>, thrice for X<sup>-</sup>.');

    w\js('ballot');
    
  //<form name='ballotform' id='ballotform' action='ballot.php' method='$METHOD' onsubmit="return checkform(this, $optCount, '$qu[type]');">

    $mid = <<<EOF
<div class='ballot well q$optType'>

<div class='ballotheader'>
<div class='electiontitle'>Proposal Grading</div electiontitle>
<div class='questioncount'>(Proposal #$qndpy of $qcount)</div questioncount>
<div class='clearer'></div>
</div ballotheader>

<div class='question'><b>PROPOSAL #$qndpy:</b> $project</div question>
<div class='questiondetail'>$detail</div questiondetail>
<div class='optionheader'>$tableHeader</div optionheader>
<div id='options'>
$opts
</div options>
</div ballot>
EOF;

    $extra['mid'] = w\item($mid);
    $extra['question'] = w\hidFld(-$proposal);
    $submit = w\submit(BARE . t('Done With Proposal #') . $qndpy, 'primary', 'md', w\clas('pull-right'));
    $extra['prev'] = w\submit('&laquo; ' . t('Previous'), 'primary', 'md', w\suffix(w\rendA('submit', $submit)));
  } elseif ($qnum === FALSE and !$mya->admManageVote) return !$err = t('That proposal is not viewable at this time.');

//  $types = ray('Investment Loan Incentive Grant Other');

  if (is_numeric(trim(mb_substr(nn($sponsor), 0, 3)))) $sponsor = mb_substr($sponsor, 3);
  foreach (ray(RECOVERY_OPTS) as $i => $v) if (u\getBit($recovery, $i)) $ways[] = $v;
  
  $rows = [
//    t('Project Title:') => $project,
    t('Categories:') => str_replace(' ', ', ', $categories),
    t('Description:') => $overview,
    t('Purpose:') => $purpose,
    t('Systemic Change:') => $systemic,
    t('Where:') => $where,
    t('When:') => t('From %start to %end.', 'start end', fmtDt($when), fmtDt($until)),
    t('Implementation Plan:') => $how,
    t('Amount Requested:') => u\fmtAmt($amount),
    t('Recovery of Funds:') => join("\n", nn($ways) ?: []),
//    t('Funding Type:') => $types[$type],
    t('Total Budget:') => u\fmtAmt($budgetTotal),
    t('Budget Details:') => $budget,
    t('Contingency Plan:') => $contingency,
    t('Qualifications:') => $qualifications,
    t('Evaluation Plan:') => $evaluation,
    t('Applicant:') => $name,
    t('Contact:') => $contact,
    t('Sponsor:') => $sponsor,
  ];
  
  if ($mya->admManageVote) $rows += [
    ' ----------' => '<small class="dim">' . t('for admin only:') . '</small>',
    t('Phone:') => $phone,
    t('Email:') => $email,
  ];
  
  $f = 0;
  foreach ($rows as $k => $v) $extra['f' . $f++] = w\item(strtr(nn($v), ["\n" => "<br>\n", '  ' => '&nbsp; ']), $k);

  return $extra;
}

/**
 * Return a summary of the proposals, as graded by the community.
 */
function showProposalGrades($eid, $noProxies) {
  extract(db\get('end, ctty', 'r_events', "id=:eid AND type='G'", compact('eid')));
  $rfpId = db\max('id', 'r_events', "id<:eid AND type='P' AND ctty=:ctty", compact(ray('eid ctty')));
  $rfpName = db\get('event', 'r_events', 'id=:rfpId', compact('rfpId'));
  $maxAmt = preg_replace('/[^0-9]/', '', $rfpName) ?: 0;
  $bestAmt = round($maxAmt/10);
  $bestTime = $end + 30 * DAY_SECS;
  $bestPct = .5; // formula below needs to be generalized if this is anything but .5
  $points = db\q('SELECT name,points FROM r_criteria')->fetchAllKeyed();
  $gradableFields = db\q('SELECT id,name FROM r_criteria WHERE NOT auto')->fetchAllKeyed();
  $gradable = [];
  foreach ($gradableFields as $id => $name) $gradable[] = "(SELECT AVG(grade) FROM r_votes v LEFT JOIN r_ballots b ON b.id=v.ballot WHERE b.question=-p.id AND v.option=-$id AND grade<>-1) AS $name,\n";
  $gradable = join('', $gradable);
  
  $avgGradeOption = '';
  $sql = <<<EOF
SELECT p.id, p.project, CONCAT_WS(', ', p.name, p.contact) AS who, 
  CONCAT(MID(p.phone,2,3), '-', MID(p.phone,5,3), '-', MID(p.phone,8), ' ', p.email) AS contact, p.amount, p.categories, 
  (SELECT COUNT(DISTINCT b.id) FROM r_ballots b LEFT JOIN r_votes v ON v.ballot=b.id WHERE b.question=-p.id AND v.grade<>-1) AS count,
  $gradable
  (SELECT COUNT(v.id) FROM r_ballots b JOIN r_votes v ON v.ballot=b.id WHERE v.text <> '' AND b.question=-p.id) AS noteCount,
  MID(p.sponsor, 1, 1) AS sponsor,
  IF(MID(`where`, 0, 5)='01301', 12, 9) AS local,
  IF(type=1, 12, 0) AS goodType,
  GREATEST(0, 12-ABS(`when`-$bestTime)/:MONTH_SECS) AS beginSoon,
  GREATEST(0, 12-ABS(until-$bestTime)/:MONTH_SECS) AS endSoon,
  12 * IF(amount<$bestAmt, amount/$bestAmt, 1-GREATEST(0, amount-$bestAmt)/($maxAmt-$bestAmt)) AS goodAmt,
  12 * IF(amount/budgetTotal<$bestPct, amount/budgetTotal/$bestPct, 1-(amount/budgetTotal-$bestPct)/(1-$bestPct)) AS budgetPct,
  12 * IF(committed/budgetTotal<$bestPct, committed/budgetTotal/$bestPct, 1-(committed/budgetTotal-$bestPct)/(1-$bestPct)) AS committedPct, 
  budgetTotal, until
FROM r_proposals p WHERE event=:rfpId
ORDER BY p.project
EOF;

  $list = [];
  $grade0 = 99999;
  $cols = ray(t('categories project count who amount grade notes'));
  $grades = ray('E E+ D- D D+ C- C C+ B- B B+ A- A A+');
  $notes0 = t('<a>notes</a>', '_a', w\atag(''));
  $q = db\q($sql, compact('rfpId'));
  $rows = [array_combine($cols, $cols)];
  while ($row = $q->fetchAssoc()) $rows[] = $row;

  foreach ($rows as $row) {
    extract($row);
    
    if ($list) {
      $sponsor = $sponsor == 'M' ? 12 : ($sponsor == 'S' ? 6 : 0); // member, sponsor, or nothing
      $q = db\q('SELECT DISTINCT b.id,isVeto FROM r_ballots b JOIN r_votes v ON v.ballot=b.id WHERE b.question=-:id', compact('id'));
      if ($vetoes = array_sum($q->fetchAllKeyed())) $count .= t(', %vetoes vetoes', compact('vetoes'));
      $grade = 0;
      $gs = '';
      foreach ($points as $k => $v) {
        $grade += $$k * $v;
        $gLtr = $grades[(int) max($$k, 0)];
        $gs .= "$k:$gLtr &nbsp; ";
      }
//      $grade = round((20*$suitable + 20*$doable + 15*$mgmt + 10*$eval + 5*$cope + 2.5*$sponsor + 2.5*$local + 5*$goodType + 2.5*$goodTime1 + 2.5*$goodTime2 + 5*$rightAmt + 5*$rightPct) / 95); // should include 5*committed percent and divide by 100
      $grade0 = $grade;
      $gradeVal = number_format($grade / 100 / 3, 3);
      $gradeLtr = $grades[intval(max(0, $grade) / 100)];
      $grade = w\popHelp("$gradeVal ($gradeLtr)", $gs);
      $amount = u\fmtAmt($amount);
      $atag = w\atag("/prox/page=ProposalReport&p=$id");
      $project = "<$atag>$project</a>";
      if ($noteCount) { //($comments = proposalComments($id)) {
        $atag = w\atag("/prox/page=ProposalNotes&p=$id");
        $notes = "<$atag>$noteCount notes</a>";
      } else $notes = '';
      $rowClass = 'row';
    } else $rowClass = 'row head';
    
    $list[intval(round($grade0, 4) * 10000 + (mt_rand() / mt_getrandmax()))] = <<<EOF
<div class="$rowClass">
  <div class="project-name col col-xs-8">$project ($count)<br>$who</div>
  <div class="amount-categories col col-xs-2">$amount<br>$categories</div>
  <div class="grade-notes col col-xs-2">$grade<br>$notes</div>
</div>
EOF;
    
//    $until = fmtDt($until);
///    debug(compact(ray('categories project who amount budgetTotal until suitable doable mgmt eval cope sponsor local goodtype goodType goodTime1 goodTime2 rightAmt rightPct grade')));
  }
  krsort($list); // highest rated proposals first
  $list = join('', $list);
  return "<div id=\"proposalGrades\">$list</div>";
}

/**
 * Interpret and record the votes selected by the user.
 * @param assoc $args: input data-slider-handle
 * @param char $type: type of vote
 */
function processvote($args, $type) {
  global $quids, $now, $ermsg;
  if (!$optionCount = $args['optionCount']) return;

  for($opti = 0; $opti < $optionCount; $opti++) {
    $isVeto = nni($args, "veto$opti") ? 1 : 0;
    if ($type != 'E') {

      $grade = ($isVeto and $type != 'R') ? VETOGRADE : nni($args, "option$opti");
      if($type == 'B') {
        $grade *= BPCT2INT; // convert penny grade (pct) to integer
      } elseif($type == 'M' and !$isVeto) $grade = round($grade * 3); // multiply letter grades by 3 to keep +/-
      if ($type == 'R') $gradeMax = nni($args, 'max', NULL);
    }
      
    list ($id, $text, $modified) = [$args["vote$opti"], nni($args, "votenote$opti"), now()];
///    debug(compact(ray('args type quids now optionCount opti isVeto type grade id text modified')));
    $info = []; foreach (ray('id grade gradeMax text isVeto modified') as $k) if (isset($$k)) $info[$k] = $$k;
    db\update('r_votes', $info, 'id');
  }
  $ballot = $args['ballot'];
  $voter = db\get('voter', 'r_ballots', 'id=:ballot', compact('ballot'));
  db\update('r_ballots', ray('id proxy modified', $ballot, $voter, now()), 'id');
}

/**
 * Return information about the current vote, including how the voter already voted.
 * If the voter has no vote record yet, create it.
 * @param assoc $extras: (RETURNED) extra fields to add to the form
 * @param int $voter: who's voting (user record ID)
 * @param int $question: question ID or negative proposal ID
 * @param char $type: question type (M=multiple-choice B=budget R=range E=essay)
 * @param char $optOrder: how to order the options
 */
function getCurrentVote($mya, &$extras, $question, $type, $optOrder) {
  $voter = $mya->id;
  if (!$ballot = db\get('id', 'r_ballots', 'question=:question AND voter=:voter', compact('question', 'voter'))) {
    $created = now();
    $ballot = db\insert('r_ballots', compact(ray('question voter created')));
  }
  
  $sql = <<<EOF
    SELECT v.id AS vote, v.option, v.grade, v.gradeMax, o.minimum AS min, o.maximum AS max, v.isVeto, v.text
    FROM r_ballots b INNER JOIN r_votes v ON v.ballot=b.id LEFT JOIN r_options o ON o.id=v.option
    WHERE b.id=:ballot ORDER BY v.displayOrder
EOF;
  $q = db\q($sql, $subs = compact('ballot'));

  $rowCount = $q->rowCount();

  $optCount = $question < 0
  ? db\count('r_criteria', 'auto=0 AND ctty=0')
  : db\count('r_options', 'question=:question', compact('question'));
  
  if($rowCount < $optCount) { // vote records not set up yet (or interrupted)
    if($rowCount) db\q('DELETE FROM r_votes WHERE ballot=:ballot', compact('ballot'));
    setupVote($extras, $voter, $question, $optOrder, $ballot);
    $q = db\q($sql, $subs);
    $optCount = $q->rowCount();
  }
  $extras['optionCount'] = w\hidFld($optCount);

  $res = [];
  for ($i = 0; $row = $q->fetchAssoc(); $i++) {
    extract($row);

    if ($type == 'R') { // only one option record for range-type votes
      foreach (['min', 'max'] as $k) $res[$k] = (int) $$k;
      $res['minDft'] = $grade;
      $res['maxDft'] = $gradeMax == BLANKGRADE ? $max : $gradeMax;
    } elseif ($type == 'M') {
      if (!$isVeto) $grade /= 3;
    } elseif ($type == 'B') {
      if (!$isVeto) $grade = ($grade == BLANKGRADE) ? 100 / $optCount : ($grade / BPCT2INT);
    }

    $res["option$i"] = $grade;
    $res["veto$i"] = $isVeto;
    $res["votenote$i"] = $text;
    
    $extras["vote$i"] = w\hidFld($vote);
//    $extras["note$i"] = w\hidFld($note);
  }
  $extras['ballot'] = w\hidFld($ballot);

  return $res + compact('optCount', 'ballot');
}

function setupVote(&$extras, $voter, $question, $optOrder, &$ballot) {
  global $sql_record_id;

  $opts = setupOptions($question, $optOrder);

  for($i = 0; $i<count($opts); $i++) {
    setdbfield("option$i", BLANKGRADE);
    setdbfield("veto$i", false);
    setdbfield("votenote$i", '');
    $option = $opts[$i];
    $grade = BLANKGRADE;
    $gradeMax = BLANKGRADE;
    $displayOrder = $i;
    $vote = db\insert('r_votes', compact(ray('ballot option grade gradeMax displayOrder')));
    $extras["vote$i"] = w\hidFld($vote);
//    $extras["note$i"] = w\hidFld('');
  }
}

/** 
 * Decide in what order to display the options this time, given the optOrder style.
 * @param $int $question: question record ID
 * @param char $optOrder: N=numeric order S=shuffle Z=reverse
 */
function setupOptions($question, $optOrder='') {
  if ($question < 0) {
    $q = db\q('SELECT -id AS `option`, displayOrder FROM r_criteria WHERE auto=0 AND ctty=0');
  } else $q = db\q('SELECT id AS `option`, displayOrder FROM r_options WHERE question=:question', compact('question'));
  $opts = [];
  while ($row = $q->fetchAssoc()) { // make a list of the options
    extract($row);
    $opts[$displayOrder ?: count($opts)] = $option;
  }

  if($optOrder == 'Z') array_reverse($opts);
  if($optOrder == 'S') shuffle($opts);
  return $opts;
}

function getOptsX($ballot, $type, $defaults) {

// RETURN the options in the order the voter is used to seeing them
//  $sql = 'SELECT o.text, o.detail FROM (r_options o INNER JOIN r_votes v ON v.option=o.id) INNER JOIN r_ballots b on b.id=v.ballot WHERE b.id=:ballot ORDER BY v.displayOrder';
  $sql = 'SELECT IFNULL(o.text, c.text) AS text, IFNULL(o.detail, c.detail) AS detail FROM r_ballots b INNER JOIN r_votes v ON v.ballot=b.id LEFT JOIN r_options o ON o.id=v.option LEFT JOIN r_criteria c ON c.id=-v.option WHERE b.id=:ballot ORDER BY v.displayOrder';
  $q = db\q($sql, compact('ballot'));
  for($opti = 0, $opts=''; $row = $q->fetchAssoc(); $opti++) {
    extract(just('text detail', $row));
    $rowclass = ($opti&1) ? 'odd' : 'even';
    $opts .= call_user_func("CG\\Vote\\getOpts$type", $opti, $rowclass, $text, u\parseUp($detail), $defaults);
  }
  return $opts;
}

function getOptsB($opti, $rowclass, $opttext, $optdetail, $defaults) {
  $slider = "slider$opti";
  $option = "option$opti";
//  $onchange = 'changepct($opti);';

  $dft = nni($defaults, $option);
  $sliderHtml = <<<EOF
<input id="$slider" data-slider-id='$slider' type="text" data-slider-min="0" data-slider-max="100" data-slider-step="1" data-slider-value="$dft" data-slider-handle="triangle" />
EOF;
  $optionHtml = w\rendA($option, w\hidFld($dft));

  $optStart = optStart($opti, $rowclass, $defaults);
  $optEnd = optEnd($opti, $rowclass, $opttext, $optdetail, $defaults);

  return <<<EOF
$optStart
<div class='sliderandpct'>
<div class='gliss'>
  <div id='sliderdiv$opti' class='sliderN'>$sliderHtml</div>
</div gliss>
<div class='pct'>$optionHtml</div pct>
<nothing />
</div sliderandpct>
$optEnd
EOF;
}

/**
 * Return HTML for a "range" type question.
 */
function getOptsR($opti, $rowclass, $opttext, $optdetail, $defaults) {
  $slider = "slider$opti";
  $option = "option$opti";
//  $onchange = 'changepct($opti);';

  extract(just('min max minDft maxDft', $defaults));
  $sliderHtml = <<<EOF
<input id="$slider" data-slider-id='$slider' type="text" data-slider-min="$min" data-slider-max="$max" data-slider-step="1" data-slider-value="[$minDft, $maxDft]" data-slider-handle="triangle" />
EOF;
  $optionHtml = w\rendA($option, w\hidFld($minDft))
              . w\rendA('max', w\hidFld($maxDft));

  $optStart = optStart($opti, $rowclass, $defaults);
  $optEnd = optEnd($opti, $rowclass, $opttext, $optdetail, $defaults);

  return <<<EOF
$optStart
<div class='sliderandpct'>
<div class='gliss'>
  <div id='sliderdiv$opti' class='sliderN'>$sliderHtml</div>
</div gliss>
<div class='pct'>$optionHtml</div pct>
<nothing />
</div sliderandpct>
$optEnd
EOF;
}

/**
 * Return HTML for an "essay" type question.
 */
function getOptsE($opti, $rowclass, $opttext, $optdetail, $defaults) {
  $votenote = "votenote$opti";  
  $votenoteHtml = w\rendA($votenote, w\areaFld(BARE, '', w\dft(nni($defaults, $votenote))));

  return <<<EOF
<div class="optdetailE">
<div id="votenotediv$opti">$votenoteHtml</div votenotediv$opti>
</div><!--optdetailE-->
EOF;
}

function getOptsY($opti, $rowclass, $opttext, $optdetail, $defaults) {
  $optStart = optStart($opti, $rowclass, $defaults);
  $optEnd = optEnd($opti, $rowclass, $opttext, $optdetail, $defaults);
  $yesnoinputs = yesnoinputs($opti, $defaults);

  return <<<EOF
$optStart
<div class='yesnoinputs'>
$yesnoinputs
</div yesnoinputs>
<nothing />
$optEnd
EOF;
}

function yesnoinputs($opti, $defaults) {
  $v = "option$opti";
  $oldval = $defaults[$v]; 
  $yChecked = $nChecked = '';
  if ($oldval != -1) {
    if ($oldval) $yChecked = ' selected'; else $nChecked = ' selected';
  }

  return <<< X
  <div id="gY$opti" class="gY$yChecked" index="$opti"></div>
  <div id="gN$opti" class="gN$nChecked" index="$opti"></div>
  <input type="hidden" id="opt$opti" name="$v" value="$oldval">
X;
}

function getOptsM($opti, $rowclass, $opttext, $optdetail, $defaults) {
  $optStart = optStart($opti, $rowclass, $defaults);
  $optEnd = optEnd($opti, $rowclass, $opttext, $optdetail, $defaults);
  $gradeinputs = gradeInputs($opti, $defaults);
  
  /*
<div class='gradeheader'><table><tr>
$gradehdr
</tr></table></div gradeheader>
*/

  return <<<EOF
$optStart
<div id='grades$opti' class='grades'>
<div class='gradeinputs'>
$gradeinputs
</div gradeinputs>
<nothing />
</div grades$opti>
$optEnd
EOF;

}

function gradeInputs($opti, $defaults) {
  $v = "option$opti";
  $oldval = $defaults[$v]; 
  $optGrade = ''; // no grade yet

  for ($grade=0, $gradeinputs=''; $grade < strlen(GRADELTRS); $grade++) {
    $ltr = substr(GRADELTRS, $grade, 1); // default display (dpy) is unadorned grade letter
    if (round($oldval) == $grade and $oldval >= 0) {  // this works even for E (0)
      $checked = ' selected';
      $dpy = ($sign = $oldval - $grade) ? $dpy = '<div>' . ($sign > 0 ? '+' : '<b>&ndash;</b>') . '</div>' : '';
      $optGrade = $grade;
    } else $dpy = $checked = '';
    $gradeinputs .= <<< X
  <div id="g$ltr$opti" class="g$ltr$checked" index="$opti">$dpy</div>
X;
  }
  $gradeinputs .= <<< X
  <input type="hidden" id="opt$opti" index="$optGrade" name="$v" value="$oldval">
X;
  return $gradeinputs;
}

function optStart($opti, $rowclass, $defaults) {
  $html = '';
  foreach (['note', 'veto'] as $k) {
    $v = "$k$opti";
    $label = $k == 'note' ? t('Comment') : t('Veto');
//    $html .= w\rendA($v, w\boxFld($v, '', "<b>$label</b>", nn($defaults[$v]), w\onchange("opts[$opti].noteClick();")));
    $html .= w\rendA($v, w\boxFld($v, '', "<b>$label</b>", nni($defaults, $v), ray('index', $opti)));
  }
  w\jsx('veto');
  return <<<EOF
<div id="#optRow$opti" class="optRow $rowclass">
<div class="veto">$html</div veto>
EOF;
}

function optEnd($opti, $rowclass, $opttext, $optdetail, $defaults) {
  global $rUrl;
  $votenote = "votenote$opti";  
  $votenoteHtml = w\rendA($votenote, w\areaFld(BARE, '', w\dft(nni($defaults, $votenote))));
  $optImgWrapClass = $optdetail ? 'optImgWrap' : 'hidden';

  return <<<EOF
<div class="item">
  <div class="$optImgWrapClass">
    <img src="$rUrl/images/rvote/expand.gif" id="expand$opti" class="optImg expand" index="$opti" alt="show detail" title="show detail" width="13" height="13" />
  </div><!--optImgWrap-->
$opttext</div><!--item-->

<div id="optdetail$opti" class="optdetail">
  <div id="votenotediv$opti" class="votenotediv">$votenoteHtml</div><!--votenotediv$opti-->
  <div class="optdetailheader">OPTION DETAIL:</div><!--optdetailheader--><div id="optdetailtext$opti" class="optdetailtext">$optdetail<br>&nbsp;</div><!--optdetailtext$opti-->
</div><!--optdetail$opti-->

</div><!--optRow$opti-->\n\n
EOF;
}

function tableHeader($type) {
  global $rUrl;
  $leftarrow = "<img src=\"$rUrl/images/rvote/arrow-left.gif\" alt='' />";
  $rightarrow = "<img src=\"$rUrl/images/rvote/arrow-right.gif\" alt='' />";

  if (FALSE and $type == 'M') return <<<EOF
<div class='grades'>$leftarrow Bad | Good $rightarrow</div>
EOF;
  return '';
}

/*
function specialhdrs($question_type) {
  global $rUrl/images/rvote;
  $ans = jscript('ballot.js');
  if($question_type == 'B') {
    $ans .= "<link rel='stylesheet' type='text/css' href='$rUrl/images/rvote/inc/slider.css' />";
    foreach(array('sliderutil.js', 'slider.js', 'questionB.js', 'overview.js') as $one) $ans .= jscript($one);
  } else $ans .= jscript('questionM.js') ;

  return $ans;
}
*/

function done($mya) {
  global $proxiesForThisVoter;

  if(ISDEMO) {
    include_once R_ROOT . '/rvote/rvote-results.inc';
    $proxyCount = db\get('COUNT (DISTINCT voter)', 'r_ballots', 'proxy=:myid', ['myid' => $mya->id]) - 1;
    
    $bodymiddle = t(<<<EOF
<h2>Thank You for Voting!</h2>

<p>Normally it is important not to show election results until the election is over. Otherwise you get a "sheep" effect &mdash; later voters favoring options popular with earlier voters. For the wisest possible results, %PROJECT voting normally fosters independent judgment by saving the results until after everyone has voted.</p>

<p>However, since this is an ongoing demo, you get to see the results ahead of time. Imagine the election is over and take a peak at the table of results below.</p>

<p>Note that you currently represent %proxyCount other %PROJECT members with your vote.</p>

<h2>RESULTS</h2>
EOF
  , compact('proxyCount'));

    return $bodymiddle . showResults($mya->community); // show all results
  }

  $bodymiddle = <<<EOF
<h2>Thank You for Voting!</h2>
<p>Results will be announced here when the polls close. In the meantime, you may feel free to change any of your votes (simply sign in and select "Democracy Events" again on the Community menu).</p>
EOF;
  return $bodymiddle;
}

/**
 * Show (to admin) a list of who voted and how they voted.
 * This helps analyze flaws in the voting system (including question language).
 * @param int $ctty: community record ID
 * @param mixed $byVoter: boolean <order first by voter (not by question)> OR int single voter's record ID
 * @param string $proxies: whether to include direct votes and proxy votes (direct, proxy, or both)
 * @return the viewable list of votes
 *
 * @todo: For nonvoters maybe return <br>(You did not vote yet and neither did your proxy or alternate proxy, because they are not members of Greenfield rCredits.) 
 */
function showVotes($ctty, $byVoter = TRUE, $proxies = 'direct') {
  $subs = compact('ctty');
  $crit = $proxies == 'direct' ? ' AND b.proxy=b.voter' : ($proxies == 'proxy' ? ' AND b.proxy<>b.voter' : '');
  if ($byVoter > 1) { // specifying a single voter
    $crit .= ' AND b.voter=:byVoter';
    $subs += compact('byVoter');
  } else {
    include_once R_ROOT . '/rvote/rvote-results.inc';
    v\showResults($ctty); // refigure results if showing more than one
  }
  
  $order = $byVoter ? 'u.fullName,q.id' : 'q.id,u.fullName';
//  $ballots = $byVoter > 1 ? '(SELECT b2.id, b1.voter, b2.question FROM r_ballots b1 INNER JOIN r_ballots b2 ON (b2.voter=b1.proxy AND b2.question=b1.question))' : 'r_ballots';
  $event = db\max('event', 'r_questions');

  $sql = <<<EOF
    SELECT DISTINCT q.id AS qId, b.id AS bId, b.voter, b.proxy, q.type AS qType, u.fullName, u.trust, b.question
    FROM r_questions q 
    INNER JOIN r_ballots b ON b.question=q.id 
    INNER JOIN users u ON u.uid=b.voter
    WHERE event=$event AND community IN (:ctty, 0)$crit ORDER BY $order
EOF;

  $qvQ = db\q($sql, $subs);
  while ($qvRow = $qvQ->fetchAssoc()) {
    extract($qvRow); // qId, bId, voter, qType, fullName, trust, question
    if ($voter != $proxy) $bId = db\get('id', 'r_ballots', 'voter=:proxy AND question=:question', compact('proxy', 'question')); // this could be incorporated into query for efficiency
    if (!nn($qId1)) $qId1 = $qId; // remember first question number
    $grade = $gradeMax = $text = $isVeto = [];
    $sql = <<<EOF
      SELECT v.grade, v.gradeMax, v.text, v.isVeto 
      FROM r_votes v INNER JOIN r_options o ON v.option=o.id 
      WHERE v.ballot=:bId ORDER BY o.id
EOF;
    $vQ = db\q($sql, compact('bId'));
    while ($vRow = $vQ->fetchAssoc()) {
      $grade[] = $vRow['grade'];
      $gradeMax[] = $vRow['gradeMax'];
      $text[] = $vRow['text'];
      $isVeto[] = $vRow['isVeto'];
/*      foreach ($vRow as $k => $v) {
        ; // $$k[] = $v not allowed, $$k[$i] = $v fails
        $i++;
      } */
    }
    $line = $texts = '';
    for ($i = 0; $i < count($grade); $i++) {
      if ($isVeto[$i] or $grade[$i] == BLANKGRADE) {
        list ($char, $color) = $isVeto[$i] ? ['V', 'veto'] : ['x', 'not-veto'];
        $line .= "<b class=\"$color;\">$char</b> &nbsp;";
      } else {
        if ($qType == 'M') $line .= v\letterGrade($grade[$i] / 3, FALSE) . ' ';
        if ($qType == 'B') $line .= number_format($grade[$i] / BPCT2INT) . '&nbsp;.&nbsp;';
      }
      
      if ($qType == 'R') $line .= ($grade[$i] == BLANKGRADE ? '' : number_format($grade[$i])) . ' - ' . number_format($gradeMax[$i]);
      if ($text[$i]) $texts .= '--- ' . $text[$i] . '<br>';
    }
    if (strpos($line, '>V')) $line .= ' (V=veto)';
    if (strpos($line, '>x')) $line .= ' (x=left blank)';
    $line = $byVoter > 1 ? ($qType == 'E' ? '' : "$qId: $line<br>") : ("$qId: $fullName ($trust) -- $line<br>" . $texts);
    $lines[] = $qId == $qId1 ? "<br>$line" : $line;
  }
  return join('', nn($lines, []));
}

/**
 * Return a list of members who haven't voted yet.
 */
function showNonVoters($ctty, $direct = TRUE) {
  $subs = compact('ctty');
  $voterFromBallots = $direct ? 'b.voter FROM r_ballots b' : 'b2.voter FROM r_ballots b2 INNER JOIN r_ballots b ON b.voter=b2.proxy';
  $sql = <<<EOF
    SELECT DISTINCT u.uid, trust FROM users u
    LEFT JOIN (
      SELECT $voterFromBallots INNER JOIN r_votes v ON v.ballot=b.id WHERE v.grade<>:BLANKGRADE
    ) bv ON bv.voter=u.uid
    WHERE u.community IN (:ctty, 0) AND bv.voter IS NULL AND !:IS_CO AND :IS_OK ORDER BY u.trust DESC
EOF;

  $q = db\q($sql, $subs);
  while ($row = $q->fetchAssoc()) {
    extract($row);
    $a = r\acct($uid);
    $lines[] = u\fmtPhone($a->phone) . ": $a->fullName<br>\n";
  }
  return join('', nn($lines, []));
}    

/**
 * Return the welcome page for voting.
 */
function votingWelcome($endDate, $proxLink) {
  include_once R_ROOT . '/rvote/rvote-results.inc';
  
  $title = t('Vote');
  $subtext = w\item(t(<<<EOF
  Before you vote, please make sure you are happy with your <%a>proxy choices</a> and adjust them as needed.<br><br>
  
  Please keep in mind that you are acting on behalf of the community. You have a responsibility to vote according to what best serves the greater good. In most cases, your interests will be aligned with those of the community. But if there is a conflict, the community comes first.
EOF
  , '_a', w\atag($proxLink)));
  
  if ($mya = r\acct() and $mya->admShowVotes) {
    $showVotes = w\submit(t('Show votes'));
    $noVote = w\submit(t('Didn\'t vote yet'));
    $noDirect = w\submit(t('Didn\'t vote directly'));
    $results = $resultsNoProx = NULL;
//    $results = @$results ? w\item(v\showResults($mya->community, $noProxies)) : w\submit(t('Results'));
//    $resultsNoProx = w\submit(t('Results (no proxies)'));
    $agree = w\hidFld(TRUE);
  } else {
    $agree = w\boxFld('agree', '', t('I agree.'), '', w\required());
    $agree = w\item(str_replace('<input ', '<input required="required" ', w\rendA('agree', $agree)));
    $showVotes = $noVote = $noDirect = $results = $resultsNoProx = NULL;
  }
  $submit = w\submit(t('Vote Now'), 'success', 'lg', w\suffix(' &nbsp; ' . t('Polls close at midnight on the night of %date.', 'date', $endDate)));
  return w\cgform(compact(ray('title showVotes noVote noDirect results resultsNoProx subtext agree submit')));
} 

/**
 * Return the welcome page for grading.
 */
function gradingWelcome($endDate, $proxLink) {
  global $base_url;

  $title = w\item(t('Grade Community Investment Proposals'));
  $subtext = w\item(t(<<<EOF
  Help grade specific proposals within our community's chosen investment priorities! Grade just one or as many as you want. Please feel free to comment; for example, if you think a proposal should be considered for an investment or loan instead of a grant. The community's chosen investment priorities are Sustainability, Small Business Development, Social Justice, Food Systems, and The Arts.<br><br>
  
  You will be asked to rate the proposal on each of seven questions (see below). The remaining 25 points are assigned automatically based on amount, timing, location, etc. We're not looking for comparative grades at this stage. For example, if all of the proposals are very much in line with the community's investment priorities, you could give them all the highest rating, for that question.<br><br>
  
  Before you begin, please make sure you are happy with your <%a>proxy choices</a> and adjust them as needed.<br><br>
  
  Please keep in mind you are acting on behalf of the community. You have a responsibility to grade the proposals according to what best serves the greater good. In most cases, your interests will be aligned with those of the community. But if there is a conflict, the community comes first.
EOF
  , '_a', w\atag($proxLink)));
  
  if ($mya = r\acct() and $mya->admManageVote) {
  /*
    $showVotes = submit(t('Show votes'));
    $noVote = submit(t('Didn\'t vote yet'));
    $noDirect = submit(t('Didn\'t vote directly'));
    $results = @$results ? item(v\showResults($mya->community, $noProxies)) : submit(t('Results'));
    $resultsNoProx = submit(t('Results (no proxies)'));
    */
    $agree = w\hidFld(TRUE);
  } else {
    $agree = w\boxFld('agree', '', t('I agree.'), '', w\required());
    $agree = w\item(str_replace('<input ', '<input required="required" ', w\rendA('agree', $agree)));
  }
  $gradeIt = w\submit(t('Grade a Proposal'), 'success', 'lg', w\suffix(' &nbsp; ' . t('Grading ends at midnight on %date.', 'date', $endDate)));
  
  $info = []; foreach (ray('title subtext agree gradeIt') as $k) if (isset($$k)) $info[$k] = $$k;
  return w\cgform($info);
} 