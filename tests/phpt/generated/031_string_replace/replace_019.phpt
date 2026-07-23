--TEST--
string replacement repetition and reversal vector 019
--FILE--
<?php
$subject = 't19xt19xt19xt19xt19xt19';
echo str_replace('t19', 'R19', $subject), ':', str_repeat('t19', 5), ':', strrev($subject);
--EXPECT--
R19xR19xR19xR19xR19xR19:t19t19t19t19t19:91tx91tx91tx91tx91tx91t
