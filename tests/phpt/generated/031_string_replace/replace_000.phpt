--TEST--
string replacement repetition and reversal vector 000
--FILE--
<?php
$subject = 't00xt00xt00';
echo str_replace('t00', 'R00', $subject), ':', str_repeat('t00', 2), ':', strrev($subject);
--EXPECT--
R00xR00xR00:t00t00:00tx00tx00t
