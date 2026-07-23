--TEST--
string replacement repetition and reversal vector 030
--FILE--
<?php
$subject = 't30xt30xt30xt30xt30';
echo str_replace('t30', 'R30', $subject), ':', str_repeat('t30', 4), ':', strrev($subject);
--EXPECT--
R30xR30xR30xR30xR30:t30t30t30t30:03tx03tx03tx03tx03t
