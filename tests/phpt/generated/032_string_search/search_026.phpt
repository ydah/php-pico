--TEST--
string search and slice vector 026
--FILE--
<?php
$text = 'gggneedle26qqq';
echo strpos($text, 'needle26'), ':', (str_contains($text, 'needle26') ? 1 : 0), ':', (str_starts_with($text, 'ggg') ? 1 : 0), ':', (str_ends_with($text, 'qqq') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle26
