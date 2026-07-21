--TEST--
operator precedence and scalar output
--FILE--
<?php
echo 1 + 2 * 3, ':', (10 <=> 3), ':', (null ?? 'fallback');
--EXPECT--
7:1:fallback
