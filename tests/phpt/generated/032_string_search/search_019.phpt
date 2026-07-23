--TEST--
string search and slice vector 019
--FILE--
<?php
$text = 'jjneedle19tttt';
echo strpos($text, 'needle19'), ':', (str_contains($text, 'needle19') ? 1 : 0), ':', (str_starts_with($text, 'jj') ? 1 : 0), ':', (str_ends_with($text, 'tttt') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle19
