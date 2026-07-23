--TEST--
string replacement repetition and reversal vector 018
--FILE--
<?php
$subject = 't18xt18xt18xt18xt18';
echo str_replace('t18', 'R18', $subject), ':', str_repeat('t18', 4), ':', strrev($subject);
--EXPECT--
R18xR18xR18xR18xR18:t18t18t18t18:81tx81tx81tx81tx81t
