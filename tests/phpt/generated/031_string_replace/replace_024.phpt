--TEST--
string replacement repetition and reversal vector 024
--FILE--
<?php
$subject = 't24xt24xt24';
echo str_replace('t24', 'R24', $subject), ':', str_repeat('t24', 2), ':', strrev($subject);
--EXPECT--
R24xR24xR24:t24t24:42tx42tx42t
