--TEST--
string replacement repetition and reversal vector 026
--FILE--
<?php
$subject = 't26xt26xt26xt26xt26';
echo str_replace('t26', 'R26', $subject), ':', str_repeat('t26', 4), ':', strrev($subject);
--EXPECT--
R26xR26xR26xR26xR26:t26t26t26t26:62tx62tx62tx62tx62t
