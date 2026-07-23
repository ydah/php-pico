--TEST--
string replacement repetition and reversal vector 003
--FILE--
<?php
$subject = 't03xt03xt03xt03xt03xt03';
echo str_replace('t03', 'R03', $subject), ':', str_repeat('t03', 5), ':', strrev($subject);
--EXPECT--
R03xR03xR03xR03xR03xR03:t03t03t03t03t03:30tx30tx30tx30tx30tx30t
