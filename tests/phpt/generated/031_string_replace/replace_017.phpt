--TEST--
string replacement repetition and reversal vector 017
--FILE--
<?php
$subject = 't17xt17xt17xt17';
echo str_replace('t17', 'R17', $subject), ':', str_repeat('t17', 3), ':', strrev($subject);
--EXPECT--
R17xR17xR17xR17:t17t17t17:71tx71tx71tx71t
