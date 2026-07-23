--TEST--
string replacement repetition and reversal vector 022
--FILE--
<?php
$subject = 't22xt22xt22xt22xt22';
echo str_replace('t22', 'R22', $subject), ':', str_repeat('t22', 4), ':', strrev($subject);
--EXPECT--
R22xR22xR22xR22xR22:t22t22t22t22:22tx22tx22tx22tx22t
