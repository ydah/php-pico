--TEST--
string replacement repetition and reversal vector 008
--FILE--
<?php
$subject = 't08xt08xt08';
echo str_replace('t08', 'R08', $subject), ':', str_repeat('t08', 2), ':', strrev($subject);
--EXPECT--
R08xR08xR08:t08t08:80tx80tx80t
