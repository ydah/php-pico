--TEST--
string replacement repetition and reversal vector 012
--FILE--
<?php
$subject = 't12xt12xt12';
echo str_replace('t12', 'R12', $subject), ':', str_repeat('t12', 2), ':', strrev($subject);
--EXPECT--
R12xR12xR12:t12t12:21tx21tx21t
