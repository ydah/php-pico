--TEST--
string search and slice vector 002
--FILE--
<?php
$text = 'cccneedle02mmm';
echo strpos($text, 'needle02'), ':', (str_contains($text, 'needle02') ? 1 : 0), ':', (str_starts_with($text, 'ccc') ? 1 : 0), ':', (str_ends_with($text, 'mmm') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle02
