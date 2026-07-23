--TEST--
string replacement repetition and reversal vector 006
--FILE--
<?php
$subject = 't06xt06xt06xt06xt06';
echo str_replace('t06', 'R06', $subject), ':', str_repeat('t06', 4), ':', strrev($subject);
--EXPECT--
R06xR06xR06xR06xR06:t06t06t06t06:60tx60tx60tx60tx60t
