--TEST--
string replacement repetition and reversal vector 029
--FILE--
<?php
$subject = 't29xt29xt29xt29';
echo str_replace('t29', 'R29', $subject), ':', str_repeat('t29', 3), ':', strrev($subject);
--EXPECT--
R29xR29xR29xR29:t29t29t29:92tx92tx92tx92t
