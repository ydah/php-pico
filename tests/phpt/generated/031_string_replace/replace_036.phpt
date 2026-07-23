--TEST--
string replacement repetition and reversal vector 036
--FILE--
<?php
$subject = 't36xt36xt36';
echo str_replace('t36', 'R36', $subject), ':', str_repeat('t36', 2), ':', strrev($subject);
--EXPECT--
R36xR36xR36:t36t36:63tx63tx63t
