--TEST--
string replacement repetition and reversal vector 021
--FILE--
<?php
$subject = 't21xt21xt21xt21';
echo str_replace('t21', 'R21', $subject), ':', str_repeat('t21', 3), ':', strrev($subject);
--EXPECT--
R21xR21xR21xR21:t21t21t21:12tx12tx12tx12t
