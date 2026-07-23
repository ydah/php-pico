--TEST--
string replacement repetition and reversal vector 002
--FILE--
<?php
$subject = 't02xt02xt02xt02xt02';
echo str_replace('t02', 'R02', $subject), ':', str_repeat('t02', 4), ':', strrev($subject);
--EXPECT--
R02xR02xR02xR02xR02:t02t02t02t02:20tx20tx20tx20tx20t
