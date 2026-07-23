--TEST--
string search and slice vector 021
--FILE--
<?php
$text = 'bneedle21ll';
echo strpos($text, 'needle21'), ':', (str_contains($text, 'needle21') ? 1 : 0), ':', (str_starts_with($text, 'b') ? 1 : 0), ':', (str_ends_with($text, 'll') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle21
