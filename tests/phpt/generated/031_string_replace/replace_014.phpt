--TEST--
string replacement repetition and reversal vector 014
--FILE--
<?php
$subject = 't14xt14xt14xt14xt14';
echo str_replace('t14', 'R14', $subject), ':', str_repeat('t14', 4), ':', strrev($subject);
--EXPECT--
R14xR14xR14xR14xR14:t14t14t14t14:41tx41tx41tx41tx41t
