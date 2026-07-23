--TEST--
string search and slice vector 023
--FILE--
<?php
$text = 'dddneedle23nnnn';
echo strpos($text, 'needle23'), ':', (str_contains($text, 'needle23') ? 1 : 0), ':', (str_starts_with($text, 'ddd') ? 1 : 0), ':', (str_ends_with($text, 'nnnn') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle23
