--TEST--
string replacement repetition and reversal vector 007
--FILE--
<?php
$subject = 't07xt07xt07xt07xt07xt07';
echo str_replace('t07', 'R07', $subject), ':', str_repeat('t07', 5), ':', strrev($subject);
--EXPECT--
R07xR07xR07xR07xR07xR07:t07t07t07t07t07:70tx70tx70tx70tx70tx70t
