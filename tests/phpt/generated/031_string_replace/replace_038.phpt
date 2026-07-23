--TEST--
string replacement repetition and reversal vector 038
--FILE--
<?php
$subject = 't38xt38xt38xt38xt38';
echo str_replace('t38', 'R38', $subject), ':', str_repeat('t38', 4), ':', strrev($subject);
--EXPECT--
R38xR38xR38xR38xR38:t38t38t38t38:83tx83tx83tx83tx83t
