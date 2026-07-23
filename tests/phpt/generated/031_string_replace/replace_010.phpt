--TEST--
string replacement repetition and reversal vector 010
--FILE--
<?php
$subject = 't10xt10xt10xt10xt10';
echo str_replace('t10', 'R10', $subject), ':', str_repeat('t10', 4), ':', strrev($subject);
--EXPECT--
R10xR10xR10xR10xR10:t10t10t10t10:01tx01tx01tx01tx01t
