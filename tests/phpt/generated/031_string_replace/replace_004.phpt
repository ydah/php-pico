--TEST--
string replacement repetition and reversal vector 004
--FILE--
<?php
$subject = 't04xt04xt04';
echo str_replace('t04', 'R04', $subject), ':', str_repeat('t04', 2), ':', strrev($subject);
--EXPECT--
R04xR04xR04:t04t04:40tx40tx40t
