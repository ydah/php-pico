--TEST--
string replacement repetition and reversal vector 013
--FILE--
<?php
$subject = 't13xt13xt13xt13';
echo str_replace('t13', 'R13', $subject), ':', str_repeat('t13', 3), ':', strrev($subject);
--EXPECT--
R13xR13xR13xR13:t13t13t13:31tx31tx31tx31t
