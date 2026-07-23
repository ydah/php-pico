--TEST--
string replacement repetition and reversal vector 028
--FILE--
<?php
$subject = 't28xt28xt28';
echo str_replace('t28', 'R28', $subject), ':', str_repeat('t28', 2), ':', strrev($subject);
--EXPECT--
R28xR28xR28:t28t28:82tx82tx82t
