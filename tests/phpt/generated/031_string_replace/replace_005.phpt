--TEST--
string replacement repetition and reversal vector 005
--FILE--
<?php
$subject = 't05xt05xt05xt05';
echo str_replace('t05', 'R05', $subject), ':', str_repeat('t05', 3), ':', strrev($subject);
--EXPECT--
R05xR05xR05xR05:t05t05t05:50tx50tx50tx50t
