--TEST--
string replacement repetition and reversal vector 032
--FILE--
<?php
$subject = 't32xt32xt32';
echo str_replace('t32', 'R32', $subject), ':', str_repeat('t32', 2), ':', strrev($subject);
--EXPECT--
R32xR32xR32:t32t32:23tx23tx23t
