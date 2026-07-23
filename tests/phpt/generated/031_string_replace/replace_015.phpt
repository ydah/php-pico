--TEST--
string replacement repetition and reversal vector 015
--FILE--
<?php
$subject = 't15xt15xt15xt15xt15xt15';
echo str_replace('t15', 'R15', $subject), ':', str_repeat('t15', 5), ':', strrev($subject);
--EXPECT--
R15xR15xR15xR15xR15xR15:t15t15t15t15t15:51tx51tx51tx51tx51tx51t
