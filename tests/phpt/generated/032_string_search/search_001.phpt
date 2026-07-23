--TEST--
string search and slice vector 001
--FILE--
<?php
$text = 'bbneedle01ll';
echo strpos($text, 'needle01'), ':', (str_contains($text, 'needle01') ? 1 : 0), ':', (str_starts_with($text, 'bb') ? 1 : 0), ':', (str_ends_with($text, 'll') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle01
