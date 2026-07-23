--TEST--
string replacement repetition and reversal vector 039
--FILE--
<?php
$subject = 't39xt39xt39xt39xt39xt39';
echo str_replace('t39', 'R39', $subject), ':', str_repeat('t39', 5), ':', strrev($subject);
--EXPECT--
R39xR39xR39xR39xR39xR39:t39t39t39t39t39:93tx93tx93tx93tx93tx93t
