--TEST--
string replacement repetition and reversal vector 031
--FILE--
<?php
$subject = 't31xt31xt31xt31xt31xt31';
echo str_replace('t31', 'R31', $subject), ':', str_repeat('t31', 5), ':', strrev($subject);
--EXPECT--
R31xR31xR31xR31xR31xR31:t31t31t31t31t31:13tx13tx13tx13tx13tx13t
