--TEST--
string replacement repetition and reversal vector 016
--FILE--
<?php
$subject = 't16xt16xt16';
echo str_replace('t16', 'R16', $subject), ':', str_repeat('t16', 2), ':', strrev($subject);
--EXPECT--
R16xR16xR16:t16t16:61tx61tx61t
