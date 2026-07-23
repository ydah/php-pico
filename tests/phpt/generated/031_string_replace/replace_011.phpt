--TEST--
string replacement repetition and reversal vector 011
--FILE--
<?php
$subject = 't11xt11xt11xt11xt11xt11';
echo str_replace('t11', 'R11', $subject), ':', str_repeat('t11', 5), ':', strrev($subject);
--EXPECT--
R11xR11xR11xR11xR11xR11:t11t11t11t11t11:11tx11tx11tx11tx11tx11t
