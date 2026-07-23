--TEST--
string search and slice vector 018
--FILE--
<?php
$text = 'ineedle18sss';
echo strpos($text, 'needle18'), ':', (str_contains($text, 'needle18') ? 1 : 0), ':', (str_starts_with($text, 'i') ? 1 : 0), ':', (str_ends_with($text, 'sss') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle18
