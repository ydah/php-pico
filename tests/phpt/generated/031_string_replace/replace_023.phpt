--TEST--
string replacement repetition and reversal vector 023
--FILE--
<?php
$subject = 't23xt23xt23xt23xt23xt23';
echo str_replace('t23', 'R23', $subject), ':', str_repeat('t23', 5), ':', strrev($subject);
--EXPECT--
R23xR23xR23xR23xR23xR23:t23t23t23t23t23:32tx32tx32tx32tx32tx32t
