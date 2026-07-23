--TEST--
string search and slice vector 022
--FILE--
<?php
$text = 'ccneedle22mmm';
echo strpos($text, 'needle22'), ':', (str_contains($text, 'needle22') ? 1 : 0), ':', (str_starts_with($text, 'cc') ? 1 : 0), ':', (str_ends_with($text, 'mmm') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle22
