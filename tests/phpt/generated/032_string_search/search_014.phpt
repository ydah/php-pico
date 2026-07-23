--TEST--
string search and slice vector 014
--FILE--
<?php
$text = 'eeeneedle14ooo';
echo strpos($text, 'needle14'), ':', (str_contains($text, 'needle14') ? 1 : 0), ':', (str_starts_with($text, 'eee') ? 1 : 0), ':', (str_ends_with($text, 'ooo') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle14
