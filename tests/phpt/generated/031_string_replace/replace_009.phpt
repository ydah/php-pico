--TEST--
string replacement repetition and reversal vector 009
--FILE--
<?php
$subject = 't09xt09xt09xt09';
echo str_replace('t09', 'R09', $subject), ':', str_repeat('t09', 3), ':', strrev($subject);
--EXPECT--
R09xR09xR09xR09:t09t09t09:90tx90tx90tx90t
