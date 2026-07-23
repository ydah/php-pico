--TEST--
string replacement repetition and reversal vector 027
--FILE--
<?php
$subject = 't27xt27xt27xt27xt27xt27';
echo str_replace('t27', 'R27', $subject), ':', str_repeat('t27', 5), ':', strrev($subject);
--EXPECT--
R27xR27xR27xR27xR27xR27:t27t27t27t27t27:72tx72tx72tx72tx72tx72t
