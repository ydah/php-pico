--TEST--
string replacement repetition and reversal vector 035
--FILE--
<?php
$subject = 't35xt35xt35xt35xt35xt35';
echo str_replace('t35', 'R35', $subject), ':', str_repeat('t35', 5), ':', strrev($subject);
--EXPECT--
R35xR35xR35xR35xR35xR35:t35t35t35t35t35:53tx53tx53tx53tx53tx53t
