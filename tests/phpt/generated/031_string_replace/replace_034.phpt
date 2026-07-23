--TEST--
string replacement repetition and reversal vector 034
--FILE--
<?php
$subject = 't34xt34xt34xt34xt34';
echo str_replace('t34', 'R34', $subject), ':', str_repeat('t34', 4), ':', strrev($subject);
--EXPECT--
R34xR34xR34xR34xR34:t34t34t34t34:43tx43tx43tx43tx43t
