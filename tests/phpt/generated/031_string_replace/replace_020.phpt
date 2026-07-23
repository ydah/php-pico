--TEST--
string replacement repetition and reversal vector 020
--FILE--
<?php
$subject = 't20xt20xt20';
echo str_replace('t20', 'R20', $subject), ':', str_repeat('t20', 2), ':', strrev($subject);
--EXPECT--
R20xR20xR20:t20t20:02tx02tx02t
