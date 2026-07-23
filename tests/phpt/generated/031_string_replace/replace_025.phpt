--TEST--
string replacement repetition and reversal vector 025
--FILE--
<?php
$subject = 't25xt25xt25xt25';
echo str_replace('t25', 'R25', $subject), ':', str_repeat('t25', 3), ':', strrev($subject);
--EXPECT--
R25xR25xR25xR25:t25t25t25:52tx52tx52tx52t
