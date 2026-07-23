--TEST--
string search and slice vector 013
--FILE--
<?php
$text = 'ddneedle13nn';
echo strpos($text, 'needle13'), ':', (str_contains($text, 'needle13') ? 1 : 0), ':', (str_starts_with($text, 'dd') ? 1 : 0), ':', (str_ends_with($text, 'nn') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle13
