--TEST--
string search and slice vector 024
--FILE--
<?php
$text = 'eneedle24o';
echo strpos($text, 'needle24'), ':', (str_contains($text, 'needle24') ? 1 : 0), ':', (str_starts_with($text, 'e') ? 1 : 0), ':', (str_ends_with($text, 'o') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle24
