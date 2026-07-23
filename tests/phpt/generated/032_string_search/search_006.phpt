--TEST--
string search and slice vector 006
--FILE--
<?php
$text = 'gneedle06qqq';
echo strpos($text, 'needle06'), ':', (str_contains($text, 'needle06') ? 1 : 0), ':', (str_starts_with($text, 'g') ? 1 : 0), ':', (str_ends_with($text, 'qqq') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle06
