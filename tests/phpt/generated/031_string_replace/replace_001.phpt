--TEST--
string replacement repetition and reversal vector 001
--FILE--
<?php
$subject = 't01xt01xt01xt01';
echo str_replace('t01', 'R01', $subject), ':', str_repeat('t01', 3), ':', strrev($subject);
--EXPECT--
R01xR01xR01xR01:t01t01t01:10tx10tx10tx10t
