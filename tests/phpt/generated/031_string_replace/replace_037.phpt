--TEST--
string replacement repetition and reversal vector 037
--FILE--
<?php
$subject = 't37xt37xt37xt37';
echo str_replace('t37', 'R37', $subject), ':', str_repeat('t37', 3), ':', strrev($subject);
--EXPECT--
R37xR37xR37xR37:t37t37t37:73tx73tx73tx73t
