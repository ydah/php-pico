--TEST--
string replacement repetition and reversal vector 033
--FILE--
<?php
$subject = 't33xt33xt33xt33';
echo str_replace('t33', 'R33', $subject), ':', str_repeat('t33', 3), ':', strrev($subject);
--EXPECT--
R33xR33xR33xR33:t33t33t33:33tx33tx33tx33t
